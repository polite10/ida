# sim/a2 testleri (sim.a2.*). Beyaz kutu: iç başlıkları (src/) görür. Karşı uç gerçek istemcidir:
# `viz/a2`'nin kodeği ve oturumu (`ida_viz_a2`; her host derlemesinde derlenir, Godot gerektirmez).
# Soket testleri yalnız 127.0.0.1'de ve işletim sisteminin verdiği portla dinler; kilit dosyası
# geçici dizindedir (sabit 47210 portuna ve gerçek ida/a2.kilit dosyasına dokunulmaz).

add_executable(ida_a2_test
  test_ana.cpp
  test_dayaniklilik.cpp
  test_destek.cpp
  test_nabiz.cpp
  test_protokol.cpp
  test_soket.cpp
  test_takvim.cpp
  test_tel.cpp
  test_uctan_uca.cpp
  test_yavas.cpp
  test_yayinci.cpp)
target_include_directories(ida_a2_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  # Zamanlama ölçütünün ortak yardımcısı (W4-K24, W4-K29): yalnız başlık; bağlama yok.
  "${PROJECT_SOURCE_DIR}/sim/kopru/test")
target_link_libraries(ida_a2_test PRIVATE ida_a2_sahte_dunya ida_viz_a2 Threads::Threads)
ida_a2_ayar(ida_a2_test)

set(_ida_a2_testleri
  # birim
  sha256 takvim_birim gelen_okuyucu son_deger halka
  # kabul 1: tel (A2-U01)
  tel_kodek tel_sira_ve_yuk
  # kabul 2: uçtan uca, gerçek istemciyle (A2-U05)
  uctan_uca_el_sikisma uctan_uca_durum_akisi uctan_uca_sinir
  # kabul 3: protokol hataları ve el sıkışma (A2-U02, U03, U04)
  protokol_baslik_hatalari protokol_sira protokol_ileti_sirasi el_sikisma_retleri zaman_asimlari
  ikinci_baglanti port_dolu yalniz_dongu_arabirimi
  # kabul 4: takvim ve SIL seyreltmesi
  takvim_bir_saat seyreltme_sil takvim_duraklat_sifirla
  # kabul 5: yavaş, donuk ve ölü görselleştirici (A2-U17, A2-U10)
  yavas_istemci yayin_suresi donuk_istemci_soket
  # kabul 6: nabız ve yeniden bağlanma (A2-U15, U14)
  nabiz yeniden_baglanma ortada_baglanma kesit_halka_degismezi ortada_baglanma_eszamanli soket_uctan_uca soket_kapaniyor
  # kabul 7: çalıştırma nesli (A2-U27)
  nesil_yeni_calistirma nesil_sifirlama_duraklatma
  # veri kaynağı arayüzü
  yayinci_sureklilik yayinci_gecersiz_cagri yayinci_tasma yayinci_kume_degisimi yayinci_a2siz
  # kabul 8: sahiplik kilidi (A2-U21)
  kilit
  # kabul 9: dayanıklılık
  rastgele_girdi rastgele_kaynak)
foreach(_ad IN LISTS _ida_a2_testleri)
  add_test(NAME sim.a2.${_ad} COMMAND ida_a2_test ${_ad})
endforeach()
# Ayrı süreçlerle kilit: ikinci süreç ida_a2_sahte'dir.
add_test(NAME sim.a2.kilit_surecler COMMAND ida_a2_test kilit_surecler "$<TARGET_FILE:ida_a2_sahte>")

# Uzun testler (Windows debug ve ASan derlemelerinde yavaşlar).
set_tests_properties(sim.a2.takvim_bir_saat sim.a2.rastgele_girdi sim.a2.rastgele_kaynak sim.a2.kilit_surecler
  sim.a2.ortada_baglanma PROPERTIES TIMEOUT 600)
# Zamanlama ölçen ve soket tamponlarını dolduran testler başkalarıyla aynı anda koşmaz.
set_tests_properties(sim.a2.yayin_suresi sim.a2.donuk_istemci_soket sim.a2.ortada_baglanma_eszamanli
  sim.a2.son_deger sim.a2.halka PROPERTIES RUN_SERIAL TRUE)
