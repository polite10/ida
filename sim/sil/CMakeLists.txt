# sim/sil — kartsız koşucu (W4-sil): ortak çekirdeği (ida_core) ve ida_link üst katmanını
# (ida_link_ust) PC'de, kartın yerine çalıştıran `ida_sil` programı, kitaplığı
# `ida_sil_kosucu` ve CTest testleri (sim.sil.*). Kullanım: sim/sil/BENIOKU.md.
#
# PC tarafı: C++20, CXX_EXTENSIONS OFF; uyarı politikası cmake/IdaAyarlar.cmake'den. Dış
# bağımlılık yok (vcpkg gerekmez); Windows'ta Winsock (ws2_32).

# Kaynak deposu commit özetinin ilk 32 biti (IDA_BOARD_HELLO.fw_git); yapılandırma anındaki
# HEAD (firmware ile aynı yöntem). Git yoksa 0.
set(IDA_SIL_GIT "0")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --short=8 HEAD
    OUTPUT_VARIABLE _ida_sil_git OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _ida_sil_git_rc
    ERROR_QUIET)
  if(_ida_sil_git_rc EQUAL 0 AND _ida_sil_git MATCHES "^[0-9a-f]+$")
    set(IDA_SIL_GIT "0x${_ida_sil_git}")
  endif()
endif()

find_package(Threads REQUIRED)

# GCC/Clang'da -Wshadow (MSVC /W4'teki C4456–C4459'un karşılığı).
function(ida_sil_ayar hedef)
  ida_ayar_uygula(${hedef})
  target_compile_definitions(${hedef} PRIVATE "IDA_SIL_GIT=${IDA_SIL_GIT}u")
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_sil_kosucu STATIC
  src/depo.cpp
  src/komut_satiri.cpp
  src/kosucu.cpp
  src/tasima.cpp
  src/turetme.cpp)
target_include_directories(ida_sil_kosucu
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sil_kosucu PUBLIC ida_core ida_link_ust ida_link)
if(WIN32)
  target_link_libraries(ida_sil_kosucu PUBLIC ws2_32)
endif()
ida_sil_ayar(ida_sil_kosucu)

add_executable(ida_sil src/main.cpp)
target_include_directories(ida_sil PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sil PRIVATE ida_sil_kosucu)
ida_sil_ayar(ida_sil)

# ---- Testler (sim.sil.*) -------------------------------------------------------------------
# Beyaz kutu: iç başlıkları (src/) görür. Ağ testleri 127.0.0.1'de boş bir portta sahte sim
# açar; koşucu aynı süreçte ayrı bir iş parçacığında koşar.
add_executable(ida_sil_test
  test/test_ana.cpp
  test/test_belirlenim.cpp
  test/test_birim.cpp
  test/test_destek.cpp
  test/test_komut.cpp
  test/test_oturum.cpp
  test/test_param.cpp
  test/test_pencere.cpp
  test/test_program.cpp)
target_include_directories(ida_sil_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_sil_test PRIVATE ida_sil_kosucu Threads::Threads)
ida_sil_ayar(ida_sil_test)

set(_ida_sil_testleri
  turetme_vektorleri   # A5 Ek D.1, D.2, D.7 vektörleri (nonce türetimi)
  crc32
  depo                 # kalıcı depo: sayaç, blok, bozuk dosya, atomik yazım
  komut_satiri
  oturum               # kabul 1
  geri_donus           # A4-K11: sim zamanının geri başlaması
  komut_yolu           # kabul 2
  param_deposu         # kabul 3 (REBOOT dahil)
  pencere              # kabul 4 (W3-K10)
  belirlenim)          # kabul 5
foreach(_ad IN LISTS _ida_sil_testleri)
  add_test(NAME sim.sil.${_ad}
    COMMAND ida_sil_test ${_ad} "${CMAKE_CURRENT_BINARY_DIR}/test_gecici")
  set_tests_properties(sim.sil.${_ad} PROPERTIES TIMEOUT 180)
endforeach()

# Program: ayrı süreç olarak bir oturum ve ikinci açılış (depo dosyası); --surum; kullanım
# hatası (çıkış 1).
add_test(NAME sim.sil.program
  COMMAND ida_sil_test program "${CMAKE_CURRENT_BINARY_DIR}/test_gecici" $<TARGET_FILE:ida_sil>)
set_tests_properties(sim.sil.program PROPERTIES TIMEOUT 180)
add_test(NAME sim.sil.program_surum COMMAND ida_sil --surum)
set_tests_properties(sim.sil.program_surum PROPERTIES PASS_REGULAR_EXPRESSION "^ida_sil ")
add_test(NAME sim.sil.program_kullanim_hatasi COMMAND ida_sil --a1-port 0)
set_tests_properties(sim.sil.program_kullanim_hatasi PROPERTIES WILL_FAIL TRUE)
