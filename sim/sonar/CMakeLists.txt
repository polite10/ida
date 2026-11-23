# İDA sonar kümesi (W4-sonar): `ida_sonar_akustik`, `ida_sonar` kitaplıkları ve CTest testleri
# (sim.sonar.*).
#
# PC tarafı: C++20, CXX_EXTENSIONS OFF; uyarı ve kayan nokta politikası cmake/IdaAyarlar.cmake'den.
# `ida_sonar_akustik` (ses hızı ve soğurma) başka İDA kitaplığına bağlı değildir; sensör kümesi de
# kullanır (W4-K1). `ida_sonar` sensör kümesinin rastgelelik, arıza ve ölçüm türlerini kullanır.

function(ida_sonar_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    # MSVC /W4'ün gölgeleme (C4456–C4459) uyarılarının karşılığı.
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_sonar_akustik STATIC src/akustik.cpp)
target_include_directories(ida_sonar_akustik PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
ida_sonar_ayar(ida_sonar_akustik)

add_library(ida_sonar STATIC
  src/ayarlar.cpp
  src/demet.cpp
  src/sahne.cpp
  src/sonar_kumesi.cpp)
target_include_directories(ida_sonar
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sonar PUBLIC ida_sensor ida_sonar_akustik)
ida_sonar_ayar(ida_sonar)

add_executable(ida_sonar_test
  test/test_ana.cpp
  test/test_akustik.cpp
  test/test_geometri.cpp
  test/test_enerji.cpp
  test/test_zaman.cpp
  test/test_rastgele.cpp
  test/test_ariza.cpp
  test/test_kurulum.cpp
  test/test_sure.cpp)
target_include_directories(ida_sonar_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sonar_test PRIVATE ida_sonar)
ida_sonar_ayar(ida_sonar_test)

set(_ida_sonar_testleri
  ses_hizi                 # kabul 3
  sogurma
  menzil_yasasi
  malzeme
  ping1d_duz_taban         # kabul 1
  ping360_kure_duvar
  ping360_yonelim_montaj
  enerji_isin_sayisi       # kabul 2
  yakinsama_mp7
  zaman_ping360            # kabul 4
  zaman_ping1d
  zaman_hareket
  belirlenimcilik          # kabul 5
  sonar_bagimsizligi
  ariza_etkileri           # kabul 6
  ariza_desenleri
  kurulum_hatalari
  w4k1_sensor_kumesi       # kabul 7
  sure)                    # kabul 8 (ölçüm; eşik yok)
foreach(_ad IN LISTS _ida_sonar_testleri)
  add_test(NAME sim.sonar.${_ad} COMMAND ida_sonar_test ${_ad})
endforeach()
