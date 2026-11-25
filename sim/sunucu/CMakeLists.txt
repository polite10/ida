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

add_library(ida_sunucu STATIC src/sunucu.cpp src/senaryo_plani.cpp src/yardimci.cpp src/a3_baglanti.cpp src/a2_baglanti.cpp
  src/sinyal.cpp)
target_include_directories(ida_sunucu
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# A5 senaryo dosyası (W4-sim-senaryo): çözümleyici ida_senaryo, yükleyiciler ida_senaryo_sensor.
target_link_libraries(ida_sunucu PUBLIC ida_kopru ida_kopru_karsi ida_fizik ida_sensor ida_senaryo ida_senaryo_sensor)
# Sonar (W4-bagla-sonar): sonar kümesi ida_sonar, senaryodan kurulum ida_senaryo_sonar (sim/sonar
# sonradan tanımlanır; hedef adları üretimde çözülür).
target_link_libraries(ida_sunucu PUBLIC ida_sonar ida_senaryo_sonar)
# A3 sunucusu (W4-bagla-a3): --a3-port verilince başlar; ida_a3 sim/a3'te tanımlanır (hedef adı üretimde çözülür).
target_link_libraries(ida_sunucu PUBLIC ida_a3 PRIVATE nlohmann_json::nlohmann_json)
# A2 sunucusu (W5-bagla-a2): --a2-port verilince başlar; ida_a2 sim/a2'de tanımlanır (hedef adı üretimde
# çözülür). PUBLIC: sunucu.hpp A2 sayaç türlerini (ida/a2/sunucu.hpp) taşır.
target_link_libraries(ida_sunucu PUBLIC ida_a2)
ida_sunucu_ayar(ida_sunucu)

add_executable(ida_sim src/main.cpp)
target_link_libraries(ida_sim PRIVATE ida_sunucu)
ida_sunucu_ayar(ida_sim)

# Yardımcı sınamaları (sim.sunucu.*): Windows argüman kaçışı (sol #14).
add_executable(ida_sunucu_test test/test_yardimci.cpp test/test_a3_alici.cpp test/test_a2_baglanti.cpp)
target_include_directories(ida_sunucu_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(ida_sunucu_test PRIVATE "IDA_DEPO_KOKU=\"${PROJECT_SOURCE_DIR}\"")
target_link_libraries(ida_sunucu_test PRIVATE ida_sunucu)
ida_sunucu_ayar(ida_sunucu_test)
add_test(NAME sim.sunucu.arguman_kacisi COMMAND ida_sunucu_test arguman_kacisi)
add_test(NAME sim.sunucu.arguman_surec COMMAND ida_sunucu_test arguman_surec)
# W4-K27 (2) (EMNİYET): A3 alıcısının tutarlı kesimi, belirlenimci iş parçacığı sırasıyla.
add_test(NAME sim.sunucu.a3_kesim COMMAND ida_sunucu_test a3_kesim)
add_test(NAME sim.sunucu.a3_duraklatma_tablo COMMAND ida_sunucu_test a3_duraklatma_tablo)
# W4-sunucu-kucuk kapsam 1: kapanış isteği bayrağı (her platform) ve sinyal işleyicileri (POSIX; fork).
add_test(NAME sim.sunucu.kapanis_bayragi COMMAND ida_sunucu_test kapanis_bayragi)
# W5-bagla-a2: A2 bağlama katmanı. Hız dönüşümü v_n = R(q_nb) · v_b (kabul 3 (a)), fizik çekirdeğiyle
# tutarlılık (kabul 3 (b)), çalıştırma tanımı (kapsam 2), gövde kimliği (W5-K32).
foreach(_ad a2_hiz_donusumu a2_fizik_tutarlilik a2_tanim a2_kimlik)
  add_test(NAME sim.sunucu.${_ad} COMMAND ida_sunucu_test ${_ad})
endforeach()
if(NOT WIN32)
  add_test(NAME sim.sunucu.sinyal_ilk COMMAND ida_sunucu_test sinyal_ilk)
  add_test(NAME sim.sunucu.sinyal_ikinci COMMAND ida_sunucu_test sinyal_ikinci)
endif()
