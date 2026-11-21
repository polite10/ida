# İDA sensör kümesi (W4-sensor): `ida_sensor` kitaplığı ve CTest testleri (sim.sensor.*).
#
# PC tarafı: C++20, CXX_EXTENSIONS OFF; uyarı ve kayan nokta politikası cmake/IdaAyarlar.cmake'den.
# Fizik anlık görüntüsünü (`ida::fizik::DurumAnlik`) yalnız başlıktan kullanır; ida_fizik'e ve
# Eigen'e bağlanmaz, bu yüzden vcpkg olmadan da derlenir. Bütünleme testi gerçek çekirdeği
# (ida_core, C17) bağlar.

function(ida_sensor_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    # MSVC /W4'ün gölgeleme (C4456–C4459) uyarılarının karşılığı.
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_sensor STATIC
  src/ayarlar.cpp
  src/modeller.cpp
  src/rastgele.cpp
  src/sensor_kumesi.cpp)
target_include_directories(ida_sensor
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/sim/fizik/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
ida_sensor_ayar(ida_sensor)

add_executable(ida_sensor_test
  test/test_ana.cpp
  test/test_ideal.cpp
  test/test_istatistik.cpp
  test/test_zaman.cpp
  test/test_gecerlilik.cpp
  test/test_rastgele.cpp
  test/test_ariza.cpp
  test/test_sure.cpp)
target_include_directories(ida_sensor_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sensor_test PRIVATE ida_sensor)
ida_sensor_ayar(ida_sensor_test)

set(_ida_sensor_testleri
  philox_bilinen_yanit     # kabul 5: A5 Ek D.2, D.3, D.6
  ideal_durgun             # kabul 1
  ideal_donus
  ideal_ivme
  ideal_dalis
  ideal_montaj
  istatistik_imu           # kabul 2 (Allan dahil)
  istatistik_digerleri
  zaman_hiz_gecikme        # kabul 3
  zaman_dvl
  gecerlilik_gnss          # kabul 4
  gecerlilik_dvl
  gecerlilik_doyma
  emniyet_geri_okuma
  sizinti
  belirlenimcilik          # kabul 5
  sensor_bagimsizligi
  ariza_etkileri           # kabul 6
  ariza_desenleri
  ariza_akis_bagimsizligi
  kurulum_hatalari
  sure)                    # kabul 8 (ölçüm; eşik yok)
foreach(_ad IN LISTS _ida_sensor_testleri)
  add_test(NAME sim.sensor.${_ad} COMMAND ida_sensor_test ${_ad})
endforeach()

# Kabul 7: üretilen IMU, manyetometre ve basınç örnekleri A4'e çevrilip gerçek çekirdeğin
# kestirimine verilir.
add_executable(ida_sensor_butunleme test/test_butunleme.cpp)
target_include_directories(ida_sensor_butunleme PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sensor_butunleme PRIVATE ida_sensor ida_core)
ida_sensor_ayar(ida_sensor_butunleme)
add_test(NAME sim.sensor.cekirdek_butunleme_durgun COMMAND ida_sensor_butunleme durgun)
add_test(NAME sim.sensor.cekirdek_butunleme_dalis COMMAND ida_sensor_butunleme dalis)
