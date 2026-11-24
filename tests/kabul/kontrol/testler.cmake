# tests/kabul/kontrol — test kayıtları: kabul_kontrol_testi(<dosya> <kısa ad> [BEKLENIYOR]).
# BEKLENIYOR: bugünkü çekirdeğe karşı düşen test (LABELS kabul_kontrol_beklenen). Ayrıntı
# CMakeLists.txt'te. Bugünkü çekirdek (is/w5-kestirim-yatay @ 7901f3f) acil durdurmada aktüatör
# çıktısını nötrlemiyor, birikimi sıfırlamıyor ve KON_ACIL_RAMPA_MS parametresi yok: on beş testin
# hepsi düşüyor (W5-K26'nın kart ölçümüyle tutarlı).

kabul_kontrol_testi(acil notr_basili BEKLENIYOR)
kabul_kontrol_testi(acil notr_kipler BEKLENIYOR)
kabul_kontrol_testi(acil notr_bayat_alim BEKLENIYOR)
kabul_kontrol_testi(acil notr_bayat_olcum BEKLENIYOR)
kabul_kontrol_testi(acil notr_red_dup BEKLENIYOR)
kabul_kontrol_testi(acil notr_red_bad BEKLENIYOR)
kabul_kontrol_testi(acil notr_inst BEKLENIYOR)
kabul_kontrol_testi(acil notr_gecersiz BEKLENIYOR)
kabul_kontrol_testi(acil notr_init BEKLENIYOR)
kabul_kontrol_testi(acil notr_init_orneksiz BEKLENIYOR)
kabul_kontrol_testi(acil birikim_yok BEKLENIYOR)
kabul_kontrol_testi(acil rampa_zarf BEKLENIYOR)
kabul_kontrol_testi(acil rampa_yeniden BEKLENIYOR)
kabul_kontrol_testi(acil rampa_parametre BEKLENIYOR)
kabul_kontrol_testi(ozellik ozellik BEKLENIYOR)
