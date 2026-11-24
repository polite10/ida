# sim/sunucu — başsız sim sunucusu `ida_sim` (W4-kopru): fizik + sensör kümesi + A1 köprüsü +
# zaman yöneticisi.
#
#   ida_sunucu   kitaplık (sunucu.hpp); bütünleme testleri (tests/butunleme) bağlar
#   ida_sim      program
#   ida_sunucu_test  yardımcı sınamaları
#
# Fizik Eigen'e (vcpkg) bağlıdır; ida_fizik yoksa (VCPKG_ROOT yok) bu dizin atlanır.

if(NOT TARGET ida_fizik OR NOT TARGET ida_senaryo_sensor)
  message(STATUS "ida_fizik ya da ida_senaryo_sensor yok: sim/sunucu atlandı")
  return()
endif()

function(ida_sunucu_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

find_package(nlohmann_json CONFIG REQUIRED)   # A3 yer istasyonu komutlarının parametreleri (W4-bagla-a3)

add_library(ida_sunucu STATIC src/sunucu.cpp src/senaryo_plani.cpp src/yardimci.cpp src/a3_baglanti.cpp)
target_include_directories(ida_sunucu
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# A5 senaryo dosyası (W4-sim-senaryo): çözümleyici ida_senaryo, yükleyiciler ida_senaryo_sensor.
target_link_libraries(ida_sunucu PUBLIC ida_kopru ida_kopru_karsi ida_fizik ida_sensor ida_senaryo ida_senaryo_sensor)
# A3 sunucusu (W4-bagla-a3): --a3-port verilince başlar; ida_a3 sim/a3'te tanımlanır (hedef adı üretimde çözülür).
target_link_libraries(ida_sunucu PUBLIC ida_a3 PRIVATE nlohmann_json::nlohmann_json)
ida_sunucu_ayar(ida_sunucu)

add_executable(ida_sim src/main.cpp)
target_link_libraries(ida_sim PRIVATE ida_sunucu)
ida_sunucu_ayar(ida_sim)

# Yardımcı sınamaları (sim.sunucu.*): Windows argüman kaçışı (sol #14).
add_executable(ida_sunucu_test test/test_yardimci.cpp)
target_include_directories(ida_sunucu_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sunucu_test PRIVATE ida_sunucu)
ida_sunucu_ayar(ida_sunucu_test)
add_test(NAME sim.sunucu.arguman_kacisi COMMAND ida_sunucu_test arguman_kacisi)
add_test(NAME sim.sunucu.arguman_surec COMMAND ida_sunucu_test arguman_surec)
