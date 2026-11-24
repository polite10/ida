# tests/zincir — kapalı çevrim düzeneği (W3-zincir, Z1): fizik çekirdeği + sade sensörler +
# gerçek ida_core + platformun en küçük taklidi, tek süreçte (ağ, protokol, kart yok).
#
#   ida_zincir          düzenek kitaplığı (zincir.hpp/.cpp)
#   ida_zincir_deneme   Z1'i koşturup CSV yazan araç
#   ida_zincir_test     CTest: zincir.z1 (kabul md.3–5)
#
# Fizik Eigen'e bağlıdır (vcpkg); vcpkg etkin değilse sim/fizik gibi bu dizin de atlanır.

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): tests/zincir atlandı")
  return()
endif()

function(ida_zincir_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_zincir STATIC zincir.cpp)
target_include_directories(ida_zincir PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# ida_fizik sim/ altında, tests/'ten sonra tanımlanır; hedef adı üretimde çözülür.
target_link_libraries(ida_zincir PUBLIC ida_fizik ida_core)
ida_zincir_ayar(ida_zincir)

add_executable(ida_zincir_deneme ida_zincir_deneme.cpp)
target_link_libraries(ida_zincir_deneme PRIVATE ida_zincir)
ida_zincir_ayar(ida_zincir_deneme)

add_executable(ida_zincir_test test_zincir.cpp)
target_link_libraries(ida_zincir_test PRIVATE ida_zincir)
ida_zincir_ayar(ida_zincir_test)

add_test(NAME zincir.z1 COMMAND ida_zincir_test)
