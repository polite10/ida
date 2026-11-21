# İDA fizik çekirdeği (W3-fizik): başsız, belirlenimci 6-SD fizik kitaplığı `ida_fizik`,
# deneme aracı `ida_fizik_deneme` ve CTest testleri (sim.fizik.*).
#
# PC tarafı: C++20, CXX_EXTENSIONS OFF, fizik double; uyarı ve kayan nokta politikası
# cmake/IdaAyarlar.cmake'den. Tek bağımlılık Eigen (kökteki vcpkg.json, sürüm sabit);
# 09 §2.6 gereği EIGEN_FAST_MATH=0. vcpkg etkin değilse (VCPKG_ROOT yok) ve bağımlılıklar
# zorunlu değilse dizin atlanır (tests/duman ile aynı politika).

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  if(IDA_BAGIMLILIK_ZORUNLU)
    message(FATAL_ERROR "sim/fizik: vcpkg etkin değil")
  endif()
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): sim/fizik atlandı")
  return()
endif()

find_package(Eigen3 CONFIG REQUIRED)

# GCC/Clang'da -Wshadow (MSVC'nin /W4'teki C4456/C4457/C4458 gölgeleme uyarılarının karşılığı).
function(ida_fizik_ayar hedef)
  ida_ayar_uygula(${hedef})
  target_compile_definitions(${hedef} PRIVATE EIGEN_FAST_MATH=0)
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_fizik STATIC
  src/dunya.cpp
  src/hidrostatik.cpp
  src/itici.cpp
  src/kutle_durumu.cpp
  src/parametreler.cpp)
target_include_directories(ida_fizik
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# Genel başlıklar Eigen içermez; Eigen yalnız iç başlıklarda.
target_link_libraries(ida_fizik PRIVATE Eigen3::Eigen)
ida_fizik_ayar(ida_fizik)

add_executable(ida_fizik_deneme deneme/ida_fizik_deneme.cpp)
target_link_libraries(ida_fizik_deneme PRIVATE ida_fizik)
ida_fizik_ayar(ida_fizik_deneme)

# Testler beyaz kutudur: iç başlıkları (src/) ve Eigen'i de görür.
add_executable(ida_fizik_test
  test/test_ana.cpp
  test/test_dunya.cpp
  test/test_duzeltme.cpp
  test/test_hidrostatik.cpp
  test/test_itici.cpp
  test/test_kutle.cpp
  test/test_sayisal.cpp)
target_include_directories(ida_fizik_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_fizik_test PRIVATE ida_fizik Eigen3::Eigen)
ida_fizik_ayar(ida_fizik_test)

set(_ida_fizik_testleri
  kutle_durumlari        # A5 Ek A test vektörleri; yüzerlik ağlarının hacmi
  ida0_toml_esitligi     # ida0_arac() = scenarios/ida0.toml
  hidrostatik_denge      # kabul 1
  serbest_sonum          # kabul 2
  son_hiz                # kabul 3
  enerji                 # kabul 4
  adim_yarilama          # kabul 5
  belirlenimcilik        # kabul 6
  itici_geometrisi       # kabul 7
  uzun_kosu              # kabul 8
  sure                   # kabul 9 (ölçüm; eşik yok)
  itici_modeli
  komut_zamanlama
  akinti
  kurulum_hatalari
  dalis_ve_yuzeye_cikis
  # sol incelemesinin karşı örnekleri (docs/gorevler/inceleme/W3-fizik-sol.md)
  itici_hizli_filtre               # bulgu 1
  esc_notr_disi_hazir_olmaz        # bulgu 2
  esc_notr_bekleme_sonrasi_hazir   # bulgu 2
  kurulum_sonluluk                 # bulgu 3
  transfer_olcek)                  # bulgu 4
foreach(_ad IN LISTS _ida_fizik_testleri)
  add_test(NAME sim.fizik.${_ad}
    COMMAND ida_fizik_test ${_ad} "${PROJECT_SOURCE_DIR}/scenarios/ida0.toml")
endforeach()
