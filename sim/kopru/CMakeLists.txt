# sim/kopru — sim sunucusunun A1 köprüsü (W4-kopru): taşıma (bellek, TCP, seri port), sim
# rolünde oturum, parti, yanıt ve olay uygulaması, operatör komutları; süreç içi karşı taraf.
#
#   ida_kopru         köprü ve taşıma (fizik bağımsız; Eigen/vcpkg gerekmez)
#   ida_kopru_karsi   süreç içi karşı taraf: ida_link üst katmanı + gerçek çekirdek (sınama ve
#                     `ida_sim --karsi ic`)
#   ida_kopru_test    CTest: sim.kopru.*
#
# PC tarafı: C++20, CXX_EXTENSIONS OFF; uyarı politikası cmake/IdaAyarlar.cmake'den.

find_package(Threads REQUIRED)

function(ida_kopru_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_kopru STATIC
  src/kopru.cpp
  src/kopru_parti.cpp
  src/kopru_sinir.cpp
  src/param_blogu.cpp
  src/tasima_bellek.cpp
  src/tasima_tcp.cpp
  src/tasima_seri.cpp)
target_include_directories(ida_kopru
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# ida_sensor: ölçüm tipleri ve A5 Ek D türetimi; ida_core: ida_core_info (A1-K14); ida_link:
# tel katmanı.
target_link_libraries(ida_kopru PUBLIC ida_sensor ida_core ida_link Threads::Threads)
if(WIN32)
  target_link_libraries(ida_kopru PUBLIC ws2_32)
endif()
ida_kopru_ayar(ida_kopru)

add_library(ida_kopru_karsi STATIC src/karsi.cpp)
target_include_directories(ida_kopru_karsi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_kopru_karsi PUBLIC ida_kopru ida_link_ust ida_core)
ida_kopru_ayar(ida_kopru_karsi)

add_executable(ida_kopru_test
  test/test_ana.cpp
  test/test_oturum.cpp
  test/test_yuva.cpp
  test/test_emniyet.cpp
  test/test_emniyet5.cpp
  test/test_emniyet4.cpp
  test/test_emniyet3.cpp
  test/test_parti.cpp
  test/test_emniyet2.cpp
  test/test_tasima.cpp)
target_include_directories(ida_kopru_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/test")
target_link_libraries(ida_kopru_test PRIVATE ida_kopru_karsi)
ida_kopru_ayar(ida_kopru_test)

set(_ida_kopru_testleri
  d7_vektorleri          # A5 Ek D.7 (A1-K114, K126)
  el_sikisma             # kabul 1
  on_bin_parti           # kabul 1, 2
  kopma_yeniden          # kabul 1
  yanlis_dialect         # kabul 1
  yuva_zamanlama         # kabul 3: L_act, geç yanıt, süre dolumu
  eszamansiz_guvenli     # kabul 3: A1-K78
  eszamansiz_ilk_parti   # W4-K6: ilk parti öncesi eşzamansız çıktı A1-K79'a sayılmaz
  olay_izin_yok          # kabul 4 (EMNİYET)
  olay_yinelenen         # kabul 4
  olay_suresi_gecmis     # kabul 4
  olay_fire_kabul        # kabul 4: yetkili durumda FIRE yalnız tüpü boşaltır
  emniyet_ornegi         # A1-K56: partide ≤ 1, kapanma önce
  emniyet_rev_durum      # W4-K4: revizyon girdi değerleriyle birlikte
  emniyet_capraz_alan    # sol #1, W4-K12 (1)
  emniyet_izin_acik      # sol #1, W4-K12 (2)
  yinelenen_yanit_olay   # sol #2, A1-K72, K118
  toplu_tutarlilik       # sol #3, Tablo 4.6b
  yeni_acilis_out_seq    # sol #4, A1-K105
  gecersiz_cerceve_etkisiz  # sol #11, A1-K109
  sim_gecikmesi  # sol B: #6 #9 #10 #12
  parti_butcesi  # sol B: #6 #9 #10 #12
  daraltici_kovasi  # sol B: #6 #9 #10 #12
  batch_seq_sarmasi  # sol B: #6 #9 #10 #12
  sil_tampon_sonraki_act  # W4-K14 ve seq tanısı
  sil_tampon_kapanis  # W4-K14 ve seq tanısı
  sil_tampon_siniri  # W4-K14 ve seq tanısı
  sira_tanisi  # W4-K14 ve seq tanısı
  yazma_sinirli  # sol B: #7 #8
  aygit_kalkti  # sol B: #7 #8
  emniyet_rev_ileri  # sol-2 EMNİYET: #1 #2 #3 #11
  reddedilen_yanit_gecmis  # sol-2 EMNİYET: #1 #2 #3 #11
  eksik_kume_tutarsiz  # sol-2 EMNİYET: #1 #2 #3 #11
  eksik_kume_gelmez  # sol-2 EMNİYET: #1 #2 #3 #11
  eksik_kume_tutarli  # sol-2 EMNİYET: #1 #2 #3 #11
  tutarsiz_yanit_canlilik  # sol-2 EMNİYET: #1 #2 #3 #11
  emniyet_gecmis_tahliyesi  # W4-K15 EMNİYET
  sayim_ihlali_sonradan_fire  # W4-K15 EMNİYET
  sayim_ihlali_kucuk_sayi  # W4-K15 EMNİYET
  sayim_ihlali_yanittan_once  # W4-K15 EMNİYET
  tam_yuvaya_yineleme  # W4-K15 EMNİYET
  sayiyi_asan_kapanis  # W4-K15 EMNİYET
  hata_donusu_canlilik  # W4-K15 EMNİYET
  eksik_kume_suresi  # W4-K15 EMNİYET
  bekleyen_kume_siniri  # W4-K15 EMNİYET
  ret_eden_olay_canlilik  # W4-K15 ek (6) (7)
  gec_kapanis_tamamlamaz  # W4-K15 ek (6) (7)
  canlilik_olay_yanit_ret  # W4-K15 ek (7) canlılık
  canlilik_eksik_kume  # W4-K15 ek (7) canlılık
  canlilik_ret_yineleme  # W4-K15 ek (7) canlılık
  canlilik_bekleyen_yineleme  # W4-K15 ek (7) canlılık
  canlilik_dogrulanan_kume  # W4-K15 ek (7) canlılık
  canlilik_aninda  # W4-K15 ek (7) canlılık
  sil_kisit_erken  # W4-K26 (1) EMNİYET
  bellek_tasima
  tcp_tasima
  pty_tasima)
foreach(_ad IN LISTS _ida_kopru_testleri)
  add_test(NAME sim.kopru.${_ad} COMMAND ida_kopru_test ${_ad})
endforeach()
