# sim/a3 — A3 sunucusu (W4-a3 dilim 1): web panelleri için tek portta HTTP/1.1 + WebSocket.
#
#   ida_a3        kitaplık (C++20); dış bağımlılık yok (HTTP/WebSocket bu dizinde; gerekçe BENIOKU.md)
#   ida_a3_sahte  kitaplığı sahte veri kaynağıyla çalıştıran program
#   ida_a3_test   CTest testleri (sim.a3.*); Node ile koşan uyum testleri node ve web/node_modules
#                 varsa koşar, yoksa atlanır.
#
# Yük kodlayıcıları ve katalog tabloları derleme zamanında docs/sozlesmeler/sema/a3-kanallar.json'dan
# üretilir (arac/uret_katalog.py; elle yazılmış alan sırası yok).

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

set(_ida_a3_katalog "${PROJECT_SOURCE_DIR}/docs/sozlesmeler/sema/a3-kanallar.json")
set(_ida_a3_komutlar "${PROJECT_SOURCE_DIR}/protocol/komutlar.json")
set(_ida_a3_uretim "${CMAKE_CURRENT_BINARY_DIR}/uretim")
add_custom_command(
  OUTPUT "${_ida_a3_uretim}/include/ida/a3/yukler.hpp" "${_ida_a3_uretim}/yukler.cpp"
  COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/arac/uret_katalog.py"
          "${_ida_a3_katalog}" "${_ida_a3_komutlar}" "${_ida_a3_uretim}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/arac/uret_katalog.py" "${_ida_a3_katalog}" "${_ida_a3_komutlar}"
  COMMENT "A3 kanal kataloğundan kodlayıcılar üretiliyor"
  VERBATIM)

# GCC/Clang'da -Wshadow (MSVC /W4'teki C4456–C4459'un karşılığı).
function(ida_a3_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_a3 STATIC
  src/cekirdek.cpp
  src/http.cpp
  src/json.cpp
  src/katalog.cpp
  src/ozet.cpp
  src/rastgele.cpp
  src/sema.cpp
  src/soket.cpp
  src/statik.cpp
  src/sunucu.cpp
  src/yayinci.cpp
  src/zaman.cpp
  "${_ida_a3_uretim}/yukler.cpp")
target_include_directories(ida_a3
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${_ida_a3_uretim}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_a3 PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(ida_a3 PUBLIC ws2_32 bcrypt)
endif()
ida_a3_ayar(ida_a3)

add_library(ida_a3_sahte_dunya STATIC sahte/sahte_dunya.cpp)
target_include_directories(ida_a3_sahte_dunya PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sahte")
target_link_libraries(ida_a3_sahte_dunya PUBLIC ida_a3)
ida_a3_ayar(ida_a3_sahte_dunya)

add_executable(ida_a3_sahte sahte/main.cpp)
target_link_libraries(ida_a3_sahte PRIVATE ida_a3_sahte_dunya)
ida_a3_ayar(ida_a3_sahte)

add_subdirectory(test)
